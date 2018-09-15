function varresult(tv, numworkers)
	held = matshare.share(tv);
	parfor i = 1:numworkers
		f = matshare.fetch('-r');
		matshare.share(f.data);
	end
	
	res = matshare.fetch('-a');
	
	for i = 1:numworkers+1
		if(~matshare.utils.compstruct(tv, res(i).data))
			error('Matshare failed because parallel results were not equal.');
		end
	end
	
	matshare.clearshm;
	
end

