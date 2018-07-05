function testparvarresult(tv, numworkers)
	mshpersistshare(tv);
	transfer = cell(numworkers,1);
	parfor i = 1:numworkers
		fetched = mshfetch('-r');
		transfer{i} = fetched.recent;
	end
	
	for i = 1:numworkers
		if(~compstruct(tv, transfer{i}))
			error('Matshare failed because parallel results were not equal.');
		end
	end
end

