x = cell(1,numworkers);
parfor i=1:numworkers
	rns = RandStream('mt19937ar', 'Seed', feature('getpid'));
	y = zeros(5,2);
	for j = 1:5
		y(j,1) = rand(rns);
		y(j,2) = randi(rns, 10);
	end
	x{i} = y;		
end