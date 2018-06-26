% test locked overwriting
fprintf('Testing locked overwriting... ');
for i = 1:100
	res = mshshare(0);
	parfor workernum = 1:numworkers
		mshlock;
		iter = mshfetch;
		mshoverwrite(iter, iter + 1);
		mshunlock;
	end

	if(res ~= numworkers)
		error('Unexpected result');
	end
end
fprintf('Test successful.\n\n');