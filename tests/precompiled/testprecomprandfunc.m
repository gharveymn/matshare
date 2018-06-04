mshpoolstartup;

fprintf('Running precompiled tests with random combinations of commands... ');
foldername = mfilefolder(mfilename);
parfor o = 1:numworkers
	precomp = load(fullfile(foldername,'res','randfunc', num2str(o), 'randfunctest.mat'));
	sample_num = 1;
	for i = 1:precomp.num_maxDepth_tests
		maxDepth = precomp.maxDepthV(i);
		for j = 1:precomp.num_maxElements_tests
			maxElements = precomp.maxElementsV(j);
			for k = 1:precomp.num_maxDims_tests
				maxDims = precomp.maxDimsV(k);
				for l = 1:precomp.num_maxChildren_tests
					maxChildren = precomp.maxChildrenV(l);
					for m = 1:precomp.num_typespec_tests
						typespec = precomp.typespecV(m);
						sample_num = testprecompwkr(precomp.rns, maxDepth, maxElements, maxDims, maxChildren, typespec, precomp.num_samples, precomp.bounds, precomp.tv, sample_num);
					end
				end
			end
		end
	end
end
mshclear
mshdetach
fprintf('Test successful.\n');