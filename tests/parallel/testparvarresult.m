function testparvarresult(tv, numworkers)
	held = matshare.share(tv);
	transfer = cell(numworkers,1);
	parfor i = 1:numworkers
		fetched = matshare.fetch('-r');
		transfer{i} = fetched.recent.data;
	end
	
	for i = 1:numworkers
		if(~compstruct(tv, transfer{i}))
			error('Matshare failed because parallel results were not equal.');
		end
	end
	
	held.clear;
	
end

