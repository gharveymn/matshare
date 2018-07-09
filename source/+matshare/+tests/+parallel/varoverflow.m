mshconfig('gc', 'off');
parfor i = 1:numworkers
	for j = 1:5000
		matshare.share(1);
	end
end