function testparvarresult(tv, numworkers)
	mshshare(tv);
	transfer = cell(numworkers,1);
	parfor i = 1:numworkers
		transfer{i} = mshfetch;
	end
	
	for i = 1:numworkers
		if(~compstruct(tv, transfer{i}))
			error('Matshare failed because parallel results were not equal.');
		end
	end
end

