function varresult(tv, numworkers)
	held = matshare.share(tv);
	transfer = cell(numworkers,1);
	parfor i = 1:numworkers
		f = matshare.fetch('-r');
		transfer{i} = f.data;
	end
	
	for i = 1:numworkers
		if(~matshare.scripts.compstruct(tv, transfer{i}))
			error('Matshare failed because parallel results were not equal.');
		end
	end
	
	held.clearshm;
	
end

