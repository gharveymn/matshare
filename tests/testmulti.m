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
toc  %Elapsed time is 2.748853 seconds
drawnow

%using shared version
tic;
a = MatShare;
a.shared.data = Data;								%place a copy of data in global memory
parfor i = 1:numel(funlist)
	sh = MatShare;
	resultpar(i) = feval(funlist{i}, sh.shared.data);
end
a.shared.data = [];                    %The shared memory is now detached from all processes and will be deleted
toc  %Elapsed time is 1.399766 seconds.

if(~any(resultpar - result))
	disp('Result is correct.');
else
	disp('Result is incorrect.');
end




