function testobjparvarresult(tv, numworkers)
	matshare(tv);
	transfer = cell(numworkers,1);
	parfor i = 1:numworkers
		transfer{i} = matshare.fetch;
	end
	
	for i = 1:numworkers
		if(~compstruct(tv, transfer{i}.data))
			error('Matshare failed because parallel results were not equal.');
		end
	end
end

