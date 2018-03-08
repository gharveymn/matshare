%% Run this file to compile getmatvar.
% The output is written to the 'bin' folder. 
%
% This program is written in the c99 standard, and so lcc is not
% supported.
%
% To use getmatvar globally, navigate to the 'HOME' tab
% on the MATLAB browser, click on 'Set Path' (underneath 'Preferences')
% and use the 'Add Folder...' dialogue to permanantly add getmatvar
% to your list of available functions.
%
% It is inadvisable to use the raw MEX function getmatvar_, so if
% you intend to move the function elsewhere just make sure you bring
% along getmatvar.m as well.
%
% Thanks for downloading!
%

doINSTALL = true;
mexmake;
