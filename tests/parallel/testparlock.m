% test locked overwriting
lents = 0;

locktestnum = 20;
parlocknumtests = 1000;

mshreset;

fprintf('Testing locked overwriting...\n');
for i = 1:parlocknumtests
	
	res = mshshare(0);
	for j = 1:locktestnum
		iter = mshfetch;
		mshsafeoverwrite(iter.recent, iter.recent + 1);
	end
	
	if(numel(res{1}) == 0 || res{1} ~= locktestnum)
		error('Unexpected result using mshsafeoverwrite with a normal variable.');
	end
	
	res = mshshare(sparse(0));
	for j = 1:locktestnum
		iter = mshfetch;
		mshsafeoverwrite(iter.recent, iter.recent + sparse(1));
	end
	
	if(numel(res{1}) == 0 || res{1} ~= locktestnum)
		error('Unexpected result using mshsafeoverwrite with a sparse variable.');
	end
	
	res{1} = mshshare(0);
	parfor workernum = 1:numworkers
		mshlock;
		iter = mshfetch;
		mshsafeoverwrite(iter.recent, iter.recent + 1);
		mshunlock;
	end

	if(numel(res{1}) == 0 || res{1} ~= numworkers)
		error('Unexpected result using mshoverwrite with a normal variable.');
	end
	
	res = mshshare(sparse(0));
	parfor workernum = 1:numworkers
		mshlock;
		iter = mshfetch;
		mshoverwrite(iter.recent, iter.recent + sparse(1));
		mshunlock;
	end

	if(numel(res{1}) == 0 || res{1} ~= numworkers)
		error('Unexpected result using mshoverwrite with a sparse variable.');
	end
	
	timestr = sprintf('Test %d of %d\n', i, parlocknumtests);
	fprintf([repmat('\b',1,lents) timestr]);
	lents = numel(timestr);
	
end
fprintf('Test successful.\n\n');