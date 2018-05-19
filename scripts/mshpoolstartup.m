usrver=ver;
[installedToolboxes{1:length(usrver)}] = deal(usrver.Name);
if(all(ismember('Parallel Computing Toolbox',installedToolboxes)))
	if(verLessThan('matlab','8.2'))
		if(~matlabpool('size'))
			matlabpool('open', 'local');
		end
		numworkers = matlabpool('size');
	else
		pp = gcp('nocreate');
		if(isempty(pp))
			pp = parpool;
		end
		numworkers = pp.NumWorkers;
	end
else
	error('No parallel computing toolbox detected, cannot continue.');
end

workerpids = cell(1, numworkers);
parfor i = 1:numworkers
	workerpids{i} = feature('getpid');
end