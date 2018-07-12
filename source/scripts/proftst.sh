matlab -nojvm -nosplash -r "cd ~/workspace/c/matshare/source;matshare.tests.testsuite;exit;" -D"valgrind --trace-children=yes --tool=callgrind --callgrind-out-file=$HOME/matshare_profile.log"
