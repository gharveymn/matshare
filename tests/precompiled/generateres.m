function generateres(paramsfolder, paramsscriptname, savefilename)

	addpath(paramsfolder);
	addpath(fullfile(mfilefolder(mfilename), '..'))

	eval(paramsscriptname);

	count = 1;
	tv = cell(num_maxDepth_tests, num_maxElements_tests, num_maxDims_tests, num_maxChildren_tests, num_typespec_tests, num_samples);

	%% result verification test
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
							tv{count} = variablegenerator(rns, maxDepth, maxElements, maxDims, maxChildren, true, typespec);						
							count = count + 1;
						end
					end
				end
			end
		end
	end
	
	save(savefilename);
end