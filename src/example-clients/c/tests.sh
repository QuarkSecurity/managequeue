echo "Running Valgrind tests"
echo "*****************************************************"

../../managequeue create /var/run/mq/example

tests=("valgrind --tool=memcheck --leak-check=yes --show-reachable=yes ./mqclient send /var/run/mq/example <<< testmsg" "valgrind --tool=memcheck --leak-check=yes --show-reachable=yes ./mqclient receive /var/run/mq/example"  )

for (( i = 0 ; i < ${#tests[@]} ; i++ ))
do
        echo "Running test $((i + 1)): ${tests[$i]}"
        echo "====================================================="
        eval ${tests[$i]} | echo -n
        echo "====================================================="
done

../../managequeue delete /var/run/mq/example
