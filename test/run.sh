# run all *.sh files
for i in $(ls *.sh); do
    if [ "$i" == "run.sh" ]; then
        continue
    fi
    echo "Running $i"
    # discard stderr and stdout
    ./$i 2>&1 > /dev/null
    if [ $? -ne 0 ]; then
        echo "Failed $i"
        exit 1
    fi
    rm -rf tmp/
done
