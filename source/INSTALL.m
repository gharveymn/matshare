%% Run this file to compile matshare.
% The output is written to the 'bin' folder. 
%
% To use getmatvar globally, navigate to the 'HOME' tab
% on the MATLAB browser, click on 'Set Path' (underneath 'Preferences')
% and use the 'Add Folder...' dialogue to permanantly add getmatvar
% to your list of available functions.
%
% It is inadvisable to use the raw MEX function matshare_. Use the
% provided entry functions prefixed with msh, or the MATLAB class MatShare.
%
% Thanks for downloading!
%

% This is hopefully temporary, but we may need a rewrite in C++ for R2018a
% if(~verLessThan('matlab','9.4'))
% 	error(['This function requires MATLAB version R2017b and below to compile. With some work, a new version will' ...
% 	'be available for R2018a and above']);
% end

OPTIONS;
mexmake;