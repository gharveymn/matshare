trials = 10000;

tic
for i = 1 : trials
	forwardchk;
end
toc

tic
for i = 1 : trials
	backwardchk;
end
toc