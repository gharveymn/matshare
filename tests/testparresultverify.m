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
						testparvarresult(tv, numworkers);
						
						
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