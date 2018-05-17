%% Essential variables
clear tv;
tv{1} = [];
tv{2} = sparse([]);
tv{3} = logical(sparse([]));
tv{4} = generatevariable(mxDOUBLE_CLASS, false, [0,1]);
tv{5} = generatevariable(mxDOUBLE_CLASS, false, [1,0]);
tv{6} = generatevariable(mxDOUBLE_CLASS, true, [0,1]);
tv{7} = generatevariable(mxDOUBLE_CLASS, true, [1,0]);
tv{8} = generatevariable(mxCHAR_CLASS, false, [0,1,0]);
tv{9} = 1;
tv{10} = [1,2];

fprintf('Testing essential variables... ');
for i = 1:10
	testparvarresult(tv{i}, numworkers);
end
fprintf('successful.\n\n');