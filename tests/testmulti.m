%Some data to shared
Data = round(10* rand(2000));

%standard non-parrellel version
if ~matlabpool('size')
	matlabpool('open', 'local');
end

%Create a list of function to perform on the matrix
funlist = {@(x)sum(x(:))};  % a simple function to highlist the comunications overhead
funlist = repmat(funlist, 1, matlabpool('size'));

%standard approach, a full copy of Data on all processes
tic;
parfor i = 1:numel(funlist)
	result(i) = feval(funlist{i}, Data);
end
toc  %Elapsed time is 0.462327 seconds
drawnow

%using shared version
tic;
mshshare(Data);								%place a copy of data in shared memory
parfor i = 1:numel(funlist)
	resultpar(i) = feval(funlist{i}, mshfetch);
	mshdetach;
end
mshdetach;
toc  %Elapsed time is 0.056462 seconds.

if(~any(resultpar - result))
	disp('Result is correct.');
else
	disp('Result is incorrect.');
end




