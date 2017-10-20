echo "Running Valgrind tests"
echo "*****************************************************"

tests=("valgrind --tool=memcheck --leak-check=yes --show-reachable=yes ./managequeue create /var/run/mq/example" "valgrind --tool=memcheck --leak-check=yes --show-reachable=yes ./managequeue delete /var/run/mq/example" "valgrind --tool=memcheck --leak-check=yes --show-reachable=yes ./managequeue create -c ../config/example.conf" "valgrind --tool=memcheck --leak-check=yes --show-reachable=yes ./managequeue delete -c ../config/example.conf" )

for (( i = 0 ; i < ${#tests[@]} ; i++ ))
do
        echo "Running test $((i + 1)): ${tests[$i]}"
        echo "====================================================="
        eval ${tests[$i]} | echo -n
        echo "====================================================="
done
