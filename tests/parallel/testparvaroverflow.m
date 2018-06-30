mshconfig('gc', 'off');
parfor i = 1:numworkers
	for j = 1:5000
		mshshare(1);
	end
end