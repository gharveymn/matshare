% test locked overwriting
locktestnum = 20;

fprintf('Testing locked overwriting... ');
for i = 1:100
	
%	res = mshshare(0);
%	for j = 1:locktestnum
%		iter = mshfetch;
%		mshsafeoverwrite(iter, iter + 1);
%	end
	
%	if(numel(res) == 0 || res ~= locktestnum)
%		error('Unexpected result.');
%	end
	
%	res = mshshare(sparse(0));
%	for j = 1:locktestnum
%		iter = mshfetch;
%		mshsafeoverwrite(iter, iter + sparse(1));
%	end
	
%	if(numel(res) == 0 || res ~= locktestnum)
%		error('Unexpected result.');
%	end
	
	res = mshshare(0);
	parfor workernum = 1:numworkers
		mshlock;
		iter = mshfetch;
		mshoverwrite(iter, iter + 1);
		mshunlock;
	end

	if(numel(res) == 0 || res ~= numworkers)
		error('Unexpected result.');
	end
	
	res = mshshare(sparse(0));
	parfor workernum = 1:numworkers
		mshlock;
		iter = mshfetch;
		mshoverwrite(iter, iter + sparse(1));
		mshunlock;
	end

	if(numel(res) == 0 || res ~= numworkers)
		error('Unexpected result.');
	end
	
end
fprintf('Test successful.\n\n');