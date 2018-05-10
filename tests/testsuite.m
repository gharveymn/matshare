%% TESTSUITE
%  Runs an automated suite of tests

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

%% Parallel pool startup
poolstartup;
observerpid = workerpids{1};

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
	testvariableresult(tv{i}, numworkers);
end
fprintf('successful.\n\n');

%% define result verification test parameters

% max depth of structs and cells
num_maxDepth_tests = 1;
min_maxDepth = 1;
max_maxDepth = 3;
maxDepthV = round(linspace(min_maxDepth, max_maxDepth, num_maxDepth_tests));

% max number of elements
num_maxElements_tests = 2;
min_maxElements = 0;
max_maxElements = 2000;
maxElementsV = round(linspace(min_maxElements, max_maxElements, num_maxElements_tests));

% max number of dimensions
num_maxDims_tests = 2;
min_maxDims = 2;
max_maxDims = 1000;
maxDimsV = round(linspace(min_maxDims, max_maxDims, num_maxDims_tests));

% max children per level in structs and cells
num_maxChildren_tests = 1;
min_maxChildren = 0;
max_maxChildren = 10;
maxChildrenV = round(linspace(min_maxChildren, max_maxChildren, num_maxChildren_tests));

% zero means randvargen can decide
num_typespec_tests = 1;
min_typespec = 0;
max_typespec = 0;
typespecV = linspace(min_typespec, max_typespec, num_typespec_tests);

% number of samples per variable generation model
num_samples = 100;

count = 1;
total_num_tests = num_maxDepth_tests*num_maxElements_tests*num_maxDims_tests*num_maxChildren_tests*num_typespec_tests;

%% result verification test
lents = 0;
fprintf('Verifying results of randomly generated variables...\n');
for i = 1:num_maxDepth_tests
	maxDepth = maxDepthV(i);
	for j = 1:num_maxElements_tests
		maxElements = maxElementsV(j);
		for k = 1:num_maxDims_tests
			maxDims = maxDimsV(k);
			for l = 1:num_maxChildren_tests
				maxChildren = maxChildrenV(l);
				for m = 1:num_typespec_tests
					typespec = typespecV(m);
					for n = 1:num_samples
						
						timestr = sprintf(['Overall test    %d of %d\n'...
							              'Depth test      %d of %d (maxDepth = %d)\n'...
						                   'Elements test   %d of %d (maxElements = %d)\n'...
						                   'Dimensions test %d of %d (maxDimensions = %d)\n'...
						                   'Children test   %d of %d (maxChildren = %d)\n'...
						                   'Typespec test   %d of %d (typespec = %d)\n'...
						                   'Sample          %d of %d\n'],...
									    count, total_num_tests,...
									    i,num_maxDepth_tests,maxDepth,...
									    j,num_maxElements_tests,maxElements,...
									    k,num_maxDims_tests,maxDims,...
									    l,num_maxChildren_tests,maxChildren,...
									    m,num_typespec_tests,typespec,...
									    n,num_samples);
						fprintf([repmat('\b',1,lents) timestr]);
						lents = numel(timestr);
						
						tv = randvargen(maxDepth, maxElements, maxDims, maxChildren, true, typespec);
						testvariableresult(tv, numworkers);
						
						
					end
					count = count + 1;
				end
			end
		end
	end
end
fprintf('successful.\n\n');


%% Define random call test parameters

% bound of clearing of data
bounds.clear_data.bound = 50;
bounds.clear_data.iter = randi(bounds.clear_data.bound);

bounds.clear_new.bound = 50;
bounds.clear_new.iter = randi(bounds.clear_new.bound);

bounds.clear_all.bound = 50;
bounds.clear_all.iter = randi(bounds.clear_all.bound);

% bound of random call to mshclear
bounds.mshclear.bound = 20;
bounds.mshclear.iter = randi(bounds.mshclear.bound);

bounds.mshdetach.bound = 100;
bounds.mshdetach.iter = randi(bounds.mshdetach.bound);

% bound of random call to mshparam to set to copy-on-write
bounds.chpar_copy.bound = 100;
bounds.chpar_copy.iter = randi(bounds.chpar_copy.bound);

% bound of random call to mshparam to set to overwrite
bounds.chpar_over.bound = 100;
bounds.chpar_over.iter = randi(bounds.chpar_over.bound);

% bound of random call to mshparam to set gc on
bounds.chpar_gc_on.bound = 50;
bounds.chpar_gc_on.iter = randi(bounds.chpar_gc_on.bound);

% bound of random call of mshparam to set gc off
bounds.chpar_gc_off.bound = 50;
bounds.chpar_gc_off.iter = randi(bounds.chpar_gc_off.bound);

% max depth of structs and cells
num_maxDepth_tests = 1;
min_maxDepth = 2;
max_maxDepth = 2;
maxDepthV = round(linspace(min_maxDepth, max_maxDepth, num_maxDepth_tests));

% max number of elements
num_maxElements_tests = 2;
min_maxElements = 0;
max_maxElements = 5;
maxElementsV = round(linspace(min_maxElements, max_maxElements, num_maxElements_tests));

% max number of dimensions
num_maxDims_tests = 2;
min_maxDims = 2;
max_maxDims = 7;
maxDimsV = round(linspace(min_maxDims, max_maxDims, num_maxDims_tests));

% max children per level in structs and cells
num_maxChildren_tests = 2;
min_maxChildren = 0;
max_maxChildren = 10;
maxChildrenV = round(linspace(min_maxChildren, max_maxChildren, num_maxChildren_tests));

% zero means randvargen can decide
num_typespec_tests = 1;
min_typespec = 0;
max_typespec = 0;
typespecV = linspace(min_typespec, max_typespec, num_typespec_tests);

% number of samples per variable generation model
num_samples = 1000;

count = 1;
total_num_tests = num_maxDepth_tests*num_maxElements_tests*num_maxDims_tests*num_maxChildren_tests*num_typespec_tests;

res = cell(1,numworkers);

%% random call combination test
lents = 0;
fprintf('Running tests with random combinations of commands...\n');
for i = 1:num_maxDepth_tests
	maxDepth = maxDepthV(i);
	for j = 1:num_maxElements_tests
		maxElements = maxElementsV(j);		
		for k = 1:num_maxDims_tests
			maxDims = maxDimsV(k);
			for l = 1:num_maxChildren_tests
				maxChildren = maxChildrenV(l);
				for m = 1:num_typespec_tests
					typespec = typespecV(m);
					timestr = sprintf(['Overall test    %d of %d\n'...
					                   'Depth test      %d of %d (maxDepth = %d)\n'...
								    'Elements test   %d of %d (maxElements = %d)\n'...
								    'Dimensions test %d of %d (maxDimensions = %d)\n'...
								    'Children test   %d of %d (maxChildren = %d)\n'...
								    'Typespec test   %d of %d (typespec = %d)\n'],...
								    count, total_num_tests,...
								    i,num_maxDepth_tests,maxDepth,...
								    j,num_maxElements_tests,maxElements,...
								    k,num_maxDims_tests,maxDims,...
								    l,num_maxChildren_tests,maxChildren,...
								    m,num_typespec_tests,typespec);
					fprintf([repmat('\b',1,lents) timestr]);
					lents = numel(timestr);
					parfor o = 1:numworkers
						testrandfunccombi(maxDepth, maxElements, maxDims, maxChildren, typespec, num_samples, bounds, observerpid);
					end
					count = count + 1;
				end
			end
		end
	end
end
mshclear
mshdetach
fprintf('successful.\n\n');
fprintf('Test successful.\n\n');

function testvariableresult(tv, numworkers)
	mshshare(tv);
	transfer = cell(numworkers,1);
	parfor i = 1:numworkers
		transfer{i} = mshfetch;
	end
	
	for i = 1:numworkers
		if(~compstruct(tv, transfer{i}))
			error('Matshare failed because parallel results were not equal.');
		end
	end
end


function testrandfunccombi(maxDepth, maxElements, maxDims, maxChildren, typespec, num_samples, bounds, observerpid)
%	thispid = feature('getpid');
	allvars = {};
%	lents = 0;
	
	for i = 1:num_samples
		try
			
			tv = randvargen(maxDepth, maxElements, maxDims, maxChildren, false, typespec);
			
			switch(randi(4))
				case 1
					mshshare(tv);
				case 2
					data = mshshare(tv);
				case 3
					[data, newvars] = mshshare(tv);
				case 4
					[data, newvars, allvars] = mshshare(tv);
			end
			
			switch(randi(4))
				case 1
					mshfetch;
				case 2
					data = mshfetch;
				case 3
					[data, newvars] = mshfetch;
				case 4
					[data, newvars, allvars] = mshfetch;
			end
			
			if(mod(bounds.mshclear.iter, i) == 0)
				if(rand < 0.75 || numel(allvars) == 0)
					mshclear;
				else
					clridx = sort(randi(numel(allvars),[2,1]));
					mshclear(allvars{clridx(1):clridx(2)});
				end
				bounds.mshclear.iter = randi(bounds.mshclear.bound);
			end
			
			
			if(mod(bounds.mshdetach.iter, i) == 0)
				mshdetach;
				bounds.mshdetach.iter = randi(bounds.mshdetach.bound);
			end
			
			
			if(mod(bounds.clear_data.iter,i) == 0)
				data = [];
				bounds.clear_data.iter = randi(bounds.clear_data.bound);
			end
			
			if(mod(bounds.clear_new.iter,i) == 0)
				newvars = [];
				bounds.clear_new.iter = randi(bounds.clear_new.bound);
			end
			
			if(mod(bounds.clear_all.iter,i) == 0)
				allvars = [];
				bounds.clear_all.iter = randi(bounds.clear_all.bound);
			end
			
			if(mod(bounds.chpar_copy.iter,i) == 0)
				mshparam('CopyOnWrite','true');
				bounds.chpar_copy.iter = randi(bounds.chpar_copy.bound);
			end
			
			if(mod(bounds.chpar_over.iter,i) == 0)
				mshparam('CopyOnWrite','false');
				bounds.chpar_over.iter = randi(bounds.chpar_over.bound);
			end
			
			if(mod(bounds.chpar_gc_on.iter,i) == 0)
				mshparam('GC','on');
				bounds.chpar_gc_on.iter = randi(bounds.chpar_gc_on.bound);
			end
			
			if(mod(bounds.chpar_gc_off.iter,i) == 0)
				mshparam('GC','off');
				bounds.chpar_gc_off.iter = randi(bounds.chpar_gc_off.bound);
			end
			
		catch mexcept
			if(~strcmp(mexcept.identifier,'matshare:InvalidTypeError'))
				rethrow(mexcept);
			end
		end
		
	end
	
end






