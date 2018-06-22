% test locked overwriting
fprintf('Testing locked overwriting... ');
for i = 1:100
	mshshare(0);
	parfor workernum = 1:numworkers
		mshlock;
		iter = mshfetch;
		mshoverwrite(iter, iter + 1);
		mshunlock;
	end
	res = mshfetch;

	if(res ~= numworkers)
		error('unexpected result');
	end
end
fprintf('Test successful.\n');