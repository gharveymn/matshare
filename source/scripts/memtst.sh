matlab -nosplash -r "cd ~/workspace/c/matshare/source;matshare.tests.testsuite;exit;" -D"valgrind --trace-children=yes --error-limit=no --num-callers=100 --tool=memcheck --leak-check=full --log-file=$HOME/matshare_memcheck.log --show-leak-kinds=all"
