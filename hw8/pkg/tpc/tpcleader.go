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
	"math/rand"
	"sync"
	"time"

	api_pb "github.com/Berkeley-CS162/tpc/api"
	"github.com/Berkeley-CS162/tpc/pkg/journal"
	tpc_pb "github.com/Berkeley-CS162/tpc/pkg/rpc"
	"github.com/golang/glog"
	"google.golang.org/grpc"
)

const (
	TPC_LEADER_TIMEOUT = time.Second
	MAX_LOG_SIZE       = 8
)

// TPCLeader is a state machine that runs the Two Phase Commit protocol.
// It does not make use of an explicit internal state but instead the
// state is implied based on how much of Put() has been executed.
type TPCLeader struct {
	name         string
	journal      journal.Journal
	mux          sync.Mutex
	manager      *MessageManager
	numFollowers int
}

// TPCLeaderConfig sets up the TPCLeader
type TPCLeaderConfig struct {
	Name        string
	JournalPath string
	Followers   []string
}

// NewTPCLeader takes a TPCLeaderConfig and creates the TPCLeader,
// including initializing the FSJournal at the provided directory and setting up
// the RPC clients and manager.
func NewTPCLeader(config TPCLeaderConfig) (*TPCLeader, error) {
	tpcJournal, err := journal.NewFileJournal(config.JournalPath)
	if err != nil {
		return nil, fmt.Errorf("error creating fs journal for tpc leader %s: %v", config.Name, err)
	}
	leader := &TPCLeader{
		name:    config.Name,
		journal: tpcJournal,
	}
	leader.manager = NewMessageManager(config.Followers)
	leader.numFollowers = len(config.Followers)
	err = leader.replayJournal()
	if err != nil {
		return nil, fmt.Errorf("error replaying tpc leader %s's journal: %v", config.Name, err)
	}
	return leader, nil
}

// replayJournal reads the transaction logs and continues where the server may
// have left off. If a transaction was incomplete, it finishes the transaction
// for the server.
//
// NOTE: this method should be called when the server is first initialized,
// i.e. when the executing thread is the only one with access to the state of
// the server.
func (l *TPCLeader) replayJournal() error {
	l.mux.Lock()
	defer l.mux.Unlock()

	if l.journal.Size() == 0 {
		glog.Infof("tpc leader %s has no journal to replay", l.name)
		return nil
	}

	glog.Infof("tpc leader %s replaying journal", l.name)
	var entryIterator *journal.EntryIterator = l.journal.NewIterator()
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
		  if the last journal log was an ACK, the leader didn't have an incomplete
		  transaction, so nothing needs to be done. This condition is for logic
		  control purpose
		*/
	} else {
		var ctx context.Context = context.Background()

		/*
		  if the last journal log was a PREPARE, the leader exited before/when
		  waiting for responses to the vote message. Since the followers can handle
		  retransmitted vote messages, we should continue by resending them and
		  logging the global decision
		*/
		if action == tpc_pb.Action_PREPARE {
			action = l.voteRequest(ctx, key, value)

			// log that the leader is sending the global message
			err := l.journal.Append(journal.Entry{
				Key:    key,
				Value:  value,
				Action: action,
			})
			if err != nil {
				return err
			}
		}
		/*
			otherwise, the last journal log was a COMMIT or ABORT, the leader exited
			before/when waiting for responses to the global message. Since the
			followers can handle retransmitted global messages, we should continue
			by resending them.

			the previous state also needs to send global messages, so we do them
			together here.
		*/
		l.globalRequest(ctx, action)

		// log that the leader has completed the operation transaction. At this point,
		// all followers should have consistent state regarding this KV pair
		err := l.journal.Append(journal.Entry{
			Key:    key,
			Value:  value,
			Action: tpc_pb.Action_ACK,
		})
		if err != nil {
			return err
		}
	}

	glog.Infof("tpc leader %s finished replaying journal", l.name)
	// since the leader has no incomplete transaction, the journal can be cleared
	l.journal.Empty()
	return nil
}

// voteRequest requests a vote from every single follower. If all followers
// return tpc_pb.Action_COMMIT, then a tpc_pb.Action_COMMIT will be returned;
// if any follower returns tpc_pb.Action_ABORT, an error, or the connection to
// which encounters an error, a tpc_pb.Action_COMMIT will be returend.
//
// NOTE: this method should only be called if the executing thread has acquired
// the mutex lock in l the TPCLeader.
func (l *TPCLeader) voteRequest(ctx context.Context, key, value string) tpc_pb.Action {
	voteMessage := tpc_pb.LeaderMsg{
		Type:   tpc_pb.MessageType_VOTE,
		Action: tpc_pb.Action_PREPARE,
		Key:    key,
		Value:  value,
	}
	var responseChannel chan *tpc_pb.Response = l.manager.SendMessage(ctx, voteMessage, false)

	numResponses := 0
	var vote tpc_pb.Action = tpc_pb.Action_COMMIT
	// iterate through the follower responses from the channel
	for responsePtr := range responseChannel {
		// if any of them is an abort, change the global vote to abort
		if responsePtr.Action == tpc_pb.Action_ABORT {
			vote = responsePtr.Action
		}
		// end the loop when responses from all followers were received
		numResponses++
		if numResponses == l.numFollowers {
			break
		}
	}
	// if the thread exits the for loop, then all followers voted commit
	return vote
}

// globalRequest requests all followers to execute an operation. In case of any
// errors, it retries until all followers reply the ACK.
//
// NOTE: this method should only be called if the execuring thread has acquired
// the mutext lock in l the TPCLeader.
func (l *TPCLeader) globalRequest(ctx context.Context, action tpc_pb.Action) {
	globalMessage := tpc_pb.LeaderMsg{
		Type:   tpc_pb.MessageType_GLOBAL,
		Action: action,
	}
	responseChannel := l.manager.SendMessage(ctx, globalMessage, true)

	var numResponses int = 0
	for responsePtr := range responseChannel {
		if responsePtr.Action == tpc_pb.Action_ABORT {
			glog.Errorf("tpc leader %s received abort response for global message", l.name)
		}
		// end the loop only when responses from all followers were received
		numResponses++
		if numResponses == l.numFollowers {
			break
		}
	}
}

// Get passes the get request to a random client.
func (l *TPCLeader) Get(ctx context.Context, key string) (string, error) {
	clientIndex := rand.Intn(l.numFollowers)
	hostname := l.manager.clients[clientIndex].hostname
	newConnection, err := grpc.Dial(hostname, grpc.WithInsecure())
	if err != nil {
		return "", fmt.Errorf("error dialing GET follower: %v", err)
	}
	defer newConnection.Close()
	client := api_pb.NewKeyValueAPIClient(newConnection)
	resp, err := client.Get(ctx, &api_pb.GetRequest{Key: key})
	glog.Infof("Forward to follower %s", hostname)
	if err != nil {
		return "", fmt.Errorf("error calling GET on follower: %v", err)
	}

	return resp.Value, nil
}

// Put begins the two phase commit protocol to update the value for a key.
// It is a single method that includes the vote phase and the global action phase,
// including all the necessary journaling.
func (l *TPCLeader) Put(ctx context.Context, key, value string) error {
	l.mux.Lock()
	defer l.mux.Unlock()

	// log that the leader is sending the vote message
	err := l.journal.Append(journal.Entry{
		Key:    key,
		Value:  value,
		Action: tpc_pb.Action_PREPARE,
	})
	if err != nil {
		return err
	}

	// ask all followers to vote, then ask them to execute the global decision
	var vote tpc_pb.Action = l.voteRequest(ctx, key, value)

	// log that the leader is sending the global message
	err = l.journal.Append(journal.Entry{
		Key:    key,
		Value:  value,
		Action: vote,
	})
	if err != nil {
		return err
	}

	l.globalRequest(ctx, vote)

	// log that the leader has completed the operation transaction. At this point,
	// all followers should have consistent state regarding this KV pair
	err = l.journal.Append(journal.Entry{
		Key:    key,
		Value:  value,
		Action: tpc_pb.Action_ACK,
	})
	if err != nil {
		return err
	}

	if l.journal.Size() > MAX_LOG_SIZE {
		l.journal.Empty()
	}
	if vote == tpc_pb.Action_ABORT {
		return fmt.Errorf("PUT failed")
	}
	return nil
}

// HandleMessage takes a message from the TPCLeader and returns an error.
func (l *TPCLeader) HandleMessage(tpc_pb.MessageType, tpc_pb.Action, string, string) (tpc_pb.Action, error) {
	return tpc_pb.Action_ABORT, fmt.Errorf("tpc leader can not handle TPC leader messages")
}
