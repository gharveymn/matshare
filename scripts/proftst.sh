matlab -nojvm -nosplash -r "cd /home/gene/workspace/c/matshare/;ap;mshtestsuite;exit;" -D"valgrind --trace-children=yes --tool=callgrind --callgrind-out-file=$HOME/matshare_profile.log"
