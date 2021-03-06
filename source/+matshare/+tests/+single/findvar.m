tic
import matshare.tests.common.*

params.verifylong;

y = 1;
count = 1;
total_num_tests = num_maxDepth_tests*num_maxElements_tests*num_maxDims_tests*num_maxChildren_tests*num_typespec_tests;

x = cell(total_num_tests,1);

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
						
						tv = vargen.variablegenerator(rns, maxDepth, maxElements, maxDims, maxChildren, true, typespec);
						x{y} = matshare.share('-n', char('a' + randi(26, 1, randi(63)) - 1), tv);
						if(~x{y}.testfind)
							error('did not find var');
						end
						
						y = y + 1;
						
					end
					count = count + 1;
				end
			end
		end
	end
end
collisions = matshare.getcols;
fprintf('Test successful.\n\n');
toc