/*
 * Copyright © 2019 University of California, Berkeley
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package tpc

import (
	"context"
	"fmt"
	"os"
	"sync"

	"github.com/Berkeley-CS162/tpc/pkg/journal"
	"github.com/Berkeley-CS162/tpc/pkg/kvstore"
	tpc_pb "github.com/Berkeley-CS162/tpc/pkg/rpc"
	"github.com/golang/glog"
)

const (
	TPC_INIT  tpcState = iota
	TPC_READY tpcState = iota
)

// tpcState describes the internal state of a TPCFollower
type tpcState int

// TPCFollower is a state machine that runs the Two Phase Commit protocol
type TPCFollower struct {
	name         string
	pendingEntry journal.Entry
	journal      journal.Journal
	kvstore      kvstore.KVStore
	state        tpcState
	mux          sync.Mutex
}

// TPCFollowerConfig sets up the TPCFollower
type TPCFollowerConfig struct {
	Name        string
	JournalPath string
	KVStoreDir  string
}

// NewTPCFollower takes a TPCFollowerConfig and creates the TPCFollower,
// including initializing the FSJournal and FSKVStore at the provided directories.
func NewTPCFollower(config TPCFollowerConfig) (*TPCFollower, error) {
	tpcJournal, err := journal.NewFileJournal(config.JournalPath)
	if err != nil {
		return nil, fmt.Errorf("error creating fs journal for tpc follower %s: %v", config.Name, err)
	}
	tpcKVStore, err := kvstore.NewFSKVStore(config.KVStoreDir)
	if err != nil {
		return nil, fmt.Errorf("error creating fs kvstore for tpc follower %s: %v", config.Name, err)
	}
	follower := &TPCFollower{
		name:    config.Name,
		journal: tpcJournal,
		kvstore: tpcKVStore,
		state:   TPC_INIT,
	}
	err = follower.replayJournal()
	if err != nil {
		return nil, fmt.Errorf("error replaying tpc follower %s's journal: %v", config.Name, err)
	}
	return follower, nil
}

// replayJournal reads the transaction logs and continues where the server may
// have left off. If a transaction was incomplete, it finishes the transaction
// for the server.
//
// NOTE: this method should be called when the server is first initialized,
// i.e. when the executing thread is the only one with access to the state of
// the server.
func (f *TPCFollower) replayJournal() error {
	f.mux.Lock()
	defer f.mux.Unlock()

	if f.journal.Size() == 0 {
		glog.Infof("tpc follower %s has no journal to replay", f.name)
		return nil
	}

	glog.Infof("tpc follower %s replaying journal", f.name)
	var entryIterator *journal.EntryIterator = f.journal.NewIterator()
	var key, value string
	var action tpc_pb.Action

	for {
		// terminate the loop when there's no more entry
		entry, err := entryIterator.Next()
		if err != nil {
			break
		}
		/*
		  since the leader should handle one operation at a time, there could be
		  at most one incompleted operation. Therefore we traverse the journal and
		  ignore the intermediate logs, until we arrive at the end of the journal
		  and have fetched the latest state of the server
		*/
		key, value, action = entry.Key, entry.Value, entry.Action
	}

	if action == tpc_pb.Action_ACK {
		/*
			  if the last journal log was an ACK, the follower didn't have an incomplete
				transaction, so it can be reset to the initial state, and its journal can
				be cleared
		*/
		f.pendingEntry = journal.Entry{}
		f.state = TPC_INIT
		f.journal.Empty()
	} else {
		/*
			if the last journal log was a PREPARE, the follower exited after receiving
			the vote request. Regardless of whether the follower returned a response,
			the leader can make a global decision. Else if the last journal log was
			an ABORT or COMMIT, the follower exited after receiving the global request.

			In either case, the follower hasn't received or responded to the leader's
			global message, so it must wait for the global message in order to continue
			on the transaction.

			Therefore, all cases here should have the follower's pending entry
			populated and its state changed to TPC_READY, to prepare for
			the leader's global request.
		*/
		f.pendingEntry.Key, f.pendingEntry.Value, f.pendingEntry.Action = key, value, action
		f.state = TPC_READY
	}

	glog.Infof("tpc follower %s finished replaying journal", f.name)
	return nil
}

// vote returns the action that f will perform on the given key-value pair.
// It stores the key-value pair to f's pendingEntry field, and changes f's state
// to TPC_READY. If either of these actions is not allowed, it returns
// tpc_pb.Action_Abort and any error encountered; otherwise, it returns
// tpc_pb.Action_Commit and nil as the error.
//
// if vote receives a re-transmitted message (e.g. the last response was lost),
// then it returns the same action as last time.
//
// NOTE: this method can only be called if the executing thread has acquired
// the mutex lock of f.
func (f *TPCFollower) vote(key, value string) (tpc_pb.Action, error) {
	// the message might be a re-transmission if f is already in the ready state
	if f.state == TPC_READY {
		// if the message carries the same key and value as the pending KV pair,
		// then return the same action as f promised last time
		if key == f.pendingEntry.Key && value == f.pendingEntry.Value {
			return f.pendingEntry.Action, nil

			// otherwise, the leader has broken its guarantee and sent message for
			// another operation before the previous one completes
		} else {
			glog.Errorf("tpc follower %s received vote messages for concurrent operations", f.name)
			os.Exit(-1)
		}
	}

	// log that the follower has been asked to vote on the KV pair
	err := f.journal.Append(journal.Entry{
		Key:    key,
		Value:  value,
		Action: tpc_pb.Action_PREPARE,
	})
	if err != nil {
		return tpc_pb.Action_ABORT, err
	}

	/*
		Since the global() method below is responsible for persisting/discarding
		the key-value of this operation, but that it only receives a tpc_pb.Action as
		input, vote() must store the key-value pair in some external variable that
		global() can access later.

		In fact, the key-value pair can be stored in f.pendingEntry, because the
		leader should serialize the operations and process them one at a time, so
		the follower should not see another vote message with a new KV pair until
		the current operation and KV pair has been finalized.
	*/
	f.pendingEntry = journal.Entry{
		Key:    key,
		Value:  value,
		Action: tpc_pb.Action_COMMIT,
	}
	f.state = TPC_READY
	return tpc_pb.Action_COMMIT, nil
}

// global finalizes the given action regarding a previously received KV pair.
// If the action is commit, it persists the KV pair; if the action is abort, it
// discards the KV pair.
//
// if global receives a re-transmitted message (e.g. the last response was lost),
// then it skips the action and return directly.
//
// NOTE: this method can only be called if the executing thread has acquired
// the mutex lock on f.
func (f *TPCFollower) global(action tpc_pb.Action) error {
	// the message might be a re-transmission if f is already in the init state
	if f.state == TPC_INIT {
		/*
		  since the method has no access to the referenced KV pair (furthermore,
		  the global message simply doesn't contain the KV pair), it cannot verify
		  whether this global message is a re-transmission of the already-handled
		  message. The only thing it can do is therefore to return early
		*/
		return nil
	} else {
		// log that the follower has received the global message
		err := f.journal.Append(journal.Entry{
			Key:    f.pendingEntry.Key,
			Value:  f.pendingEntry.Value,
			Action: action,
		})
		if err != nil {
			/*
			  if we return an error here, the leader will consider it as an ACK and
			  move onto the next operation, sending a new vote message while this
			  previous KV pair hasn't been settled, which would cause this follower
			  to be stuck in the ready state forever. Instead, we kill the follower
			  here, and with the leader keep on sending the global message, hopefully
			  the journal replay could resolve the issue
			*/
			glog.Errorf("tpc follower %s failed to log to journal", f.name)
			os.Exit(-1)
		}

		if action == tpc_pb.Action_ABORT {
			// if the global message is to abort, no persistence is needed
		} else if action == tpc_pb.Action_COMMIT {
			// otherwise, persist the pair to the KV store
			err = f.kvstore.Put(f.pendingEntry.Key, f.pendingEntry.Value)
			if err != nil {
				/*
				  if there's a failure in persisting to KV store, terminate the thread
				  and rely on the retry to resolve it, rather than returning an error
				  and thereby completing the transaction
				*/
				glog.Errorf("tpc follower %s cannot persist to KV store: %v", f.name, err)
				os.Exit(-1)
			}
		}

		/*
			  log that the follower has completed the operation transaction; for the
			  same reason as above, in case of a logging error, we terminate the follower
				thread and re-apply the global message, instead of returning an error and
				thereby completing the transaction
		*/
		err = f.journal.Append(journal.Entry{
			Key:    f.pendingEntry.Key,
			Value:  f.pendingEntry.Value,
			Action: tpc_pb.Action_ACK,
		})
		if err != nil {
			glog.Errorf("tpc follower %s failed to log to journal", f.name)
			os.Exit(-1)
		}

		f.pendingEntry = journal.Entry{}
		f.state = TPC_INIT
		return nil
	}
}

// Get returns the current value of a given key, if it exists.
func (f *TPCFollower) Get(ctx context.Context, key string) (string, error) {
	f.mux.Lock()
	defer f.mux.Unlock()
	val, err := f.kvstore.Get(key)
	if err != nil {
		return "", err
	}
	return val, nil
}

// Put is not supported by the TPCFollower, as PUT requests must go through the leader.
func (f *TPCFollower) Put(ctx context.Context, key, value string) error {
	return fmt.Errorf("tpc follower %s cannot PUT", f.name)
}

// HandleMessage takes a message from the TPCLeader and either calls vote or global, depending on the message type.
func (f *TPCFollower) HandleMessage(msgType tpc_pb.MessageType, action tpc_pb.Action, key, value string) (tpc_pb.Action, error) {
	f.mux.Lock()
	defer f.mux.Unlock()

	if msgType == tpc_pb.MessageType_VOTE {
		vote, err := f.vote(key, value)
		if err != nil {
			glog.Errorf("Aborting: %v", err)
			return tpc_pb.Action_ABORT, fmt.Errorf("error voting: %v", err)
		}
		return vote, nil
	} else if msgType == tpc_pb.MessageType_GLOBAL {
		err := f.global(action)
		if err != nil {
			return tpc_pb.Action_ABORT, fmt.Errorf("error commiting global: %v", err)
		}

		if f.journal.Size() > MAX_LOG_SIZE {
			f.journal.Empty()
		}
		return tpc_pb.Action_ACK, nil
	}
	return tpc_pb.Action_ABORT, fmt.Errorf("invalid message type: %v", msgType)
}
