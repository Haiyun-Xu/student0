./lwords gutenberg/sawyer.txt gutenberg/peter.txt gutenberg/time.txt gutenberg/alice.txt gutenberg/metamorphosis.txt > answer.txt;
./pwords gutenberg/sawyer.txt gutenberg/peter.txt gutenberg/time.txt gutenberg/alice.txt gutenberg/metamorphosis.txt > result.txt;

answerCount=`wc -l < answer.txt`;
resultCount=`wc -l < result.txt`;

echo "The answer has $answerCount lines and the result has $resultCount."

./comparator answer.txt result.txt
