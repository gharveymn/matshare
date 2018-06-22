% test locked overwriting
fprintf('Testing locked overwriting... ');
for i = 1:100
	mshshare([0,0]);
	parfor workernum = 1:numworkers
		mshlock;
		iter = mshfetch;
		mshoverwrite(iter, iter + 1);
		mshunlock;
	end
	res = mshfetch;

	if(res(1) ~= numworkers)
		error('unexpected result');
	end
end
fprintf('Test successful.\n');