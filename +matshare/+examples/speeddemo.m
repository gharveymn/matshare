% This demonstrates how matshare performs when feeding data to workers.

matshare.examples.mshpoolstartup;

% Some data to shared
Data = round(10* rand(2000));

%Create a list of function to perform on the matrix
funlist = {@(x)sum(x(:))};  % a simple function to highlist the comunications overhead
funlist = repmat(funlist, 1, numworkers);

%standard approach, a full copy of Data on all processes
tic
parfor i = 1:numel(funlist)
	result(i) = feval(funlist{i}, Data);
end
toc

% using shared version
tic
matshare.share(Data); % place a copy of data in shared memory
parfor i = 1:numel(funlist)
	d = matshare.fetch('-r');
	resultpar(i) = feval(funlist{i}, d.data);
end
toc  %Elapsed time will be less.

if(~any(resultpar - result))
	disp('Result is correct.');
else
	disp('Result is incorrect.');
end