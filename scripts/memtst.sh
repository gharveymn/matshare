matlab -nojvm -nosplash -r "cd /home/gene/workspace/c/matshare/;ap;testrandvars;exit;" -D"valgrind --error-limit=no --num-callers=100 --tool=memcheck --leak-check=full --log-file=/home/gene/workspace/c/matshare/res/mem.log --show-leak-kinds=all"
