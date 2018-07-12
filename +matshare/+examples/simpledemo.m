mshpoolstartup;
matshare.clear;

% get the process ids of your parpool workers
parfor i = 1:numworkers
	matshare.share(feature('getpid'));
end
f = matshare.fetch;

for i = 1:numworkers
	disp(['PID of worker ' num2str(i) ': ' num2str(f.all(i).data)]);
end