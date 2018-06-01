%% Type classes
mxUNKNOWN_CLASS  = 0;
mxCELL_CLASS     = 1;
mxSTRUCT_CLASS   = 2;
mxLOGICAL_CLASS  = 3;
mxCHAR_CLASS     = 4;
mxVOID_CLASS     = 5;
mxDOUBLE_CLASS   = 6;
mxSINGLE_CLASS   = 7;
mxINT8_CLASS     = 8;
mxUINT8_CLASS    = 9;
mxINT16_CLASS    = 10;
mxUINT16_CLASS   = 11;
mxINT32_CLASS    = 12;
mxUINT32_CLASS   = 13;
mxINT64_CLASS    = 14;
mxUINT64_CLASS   = 15;
mxFUNCTION_CLASS = 16;
mxOPAQUE_CLASS   = 17;
mxOBJECT_CLASS   = 18;

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
fprintf('Test successful.\n\n');