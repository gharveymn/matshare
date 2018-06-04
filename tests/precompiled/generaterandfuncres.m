mshpoolstartup;

paramfolder = fullfile(mfilefolder(mfilename),'..','params','randfunc');
mkdirifnotexist(paramfolder);

savefilename = fullfile(mfilefolder(mfilename),'res','randfunc');

mkdirifnotexist(paramfolder);

parfor i = 1:numworkers
	procsavefilename = fullfile(savefilename, num2str(i), 'randfunctest.mat');
	mkdirifnotexist(fileparts(procsavefilename));
	generateres(paramfolder, 'randfuncparams', procsavefilename);
end