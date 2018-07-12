% test locked overwriting
lents = 0;

locktestnum = 20;
parlocknumtests = 100;

matshare.reset;

fprintf('Testing locked overwriting...\n');
for i = 1:parlocknumtests
	
	res = matshare.share(0);
	for j = 1:locktestnum
		iter = matshare.fetch('-r');
		iter.overwrite(iter.data + 1, '-s');
	end
	
	if(numel(res.data) == 0 || res.data ~= locktestnum)
		error('Unexpected result using matshare.overwrite with a normal variable.');
	end
	
	res = matshare.share(sparse(0));
	for j = 1:locktestnum
		iter = matshare.fetch('-r');
		iter.overwrite(iter.data + sparse(1), '-s');
	end
	
	if(numel(res.data) == 0 || res.data ~= locktestnum)
		error('Unexpected result using matshare.overwrite with a sparse variable.');
	end
	
	res = matshare.share(0);
	parfor workernum = 1:numworkers
		matshare.lock;
		iter = matshare.fetch('-r');
		iter.overwrite(iter.data + 1);
		matshare.unlock;
	end

	if(numel(res.data) == 0 || res.data ~= numworkers)
		error('Unexpected result using unsafe matshare.overwrite with a normal variable.');
	end
	
	res = matshare.share(sparse(0));
	parfor workernum = 1:numworkers
		matshare.lock;
		iter = matshare.fetch('-r');
		iter.overwrite(iter.data + sparse(1));
		matshare.unlock;
	end

	if(numel(res.data) == 0 || res.data ~= numworkers)
		error('Unexpected result using unsafe matshare.overwrite with a sparse variable.');
	end
	
	timestr = sprintf('Test %d of %d\n', i, parlocknumtests);
	fprintf([repmat('\b',1,lents) timestr]);
	lents = numel(timestr);
	
end
fprintf('Test successful.\n\n');