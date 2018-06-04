paramfolder = fullfile(mfilefolder(mfilename),'..','params','verify');
savefilename = fullfile(mfilefolder(mfilename),'res','verify','verifytest.mat');

mkdirifnotexist(paramfolder);
mkdirifnotexist(fileparts(savefilename));

generateres(paramfolder, 'verifyparams', savefilename);