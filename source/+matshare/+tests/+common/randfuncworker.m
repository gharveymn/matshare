function rns = randfuncworker(maxDepth, maxElements, maxDims, maxChildren, typespec, num_samples, bounds, isparallel)

	thispid = feature('getpid');
	if(isparallel)
		% only do this if in parallel so we have consistent results otherwise
		%rns = RandStream('mt19937ar', 'Seed', mod(now*10^10, thispid));
		rns = RandStream('mt19937ar', 'Seed', thispid);
	else
		rns = RandStream('mt19937ar');
	end
%	lents = 0

	allvars = {};
	
	% unravel struct for faster access
	clear_data_bound = bounds.clear_data;
	clear_new_bound = bounds.clear_new;
	clear_all_bound = bounds.clear_all;
	mshreset_bound = bounds.mshreset;
	mshlocalcopy_bound = bounds.mshlocalcopy;
	mshclear_bound = bounds.mshclear;
	mshdetach_bound = bounds.mshdetach;
	chpar_gc_on_bound = bounds.chpar_gc_on;
	chpar_gc_off_bound = bounds.chpar_gc_off;

	% bound of clearing of data
	clear_data_iter = randi(rns, clear_data_bound);

	clear_new_iter = randi(rns, clear_new_bound);

	clear_all_iter = randi(rns, clear_all_bound);
	
	mshreset_iter = randi(rns, mshreset_bound);

	mshlocalcopy_iter = randi(rns, mshlocalcopy_bound);
	
	% bound of random call to mshclear
	mshclear_iter = randi(rns, mshclear_bound);

	mshdetach_iter = randi(rns, mshdetach_bound);

	% bound of random call to mshconfig to set gc on
	chpar_gc_on_iter = randi(rns, chpar_gc_on_bound);

	% bound of random call of mshconfig to set gc off
	chpar_gc_off_iter = randi(rns, chpar_gc_off_bound);
	
	randdoubles1 = rand(rns,[num_samples,1]);
	randdoubles2 = rand(rns,[num_samples,1]);
	randdoubles3 = rand(rns,[num_samples,1]);
	
	data = [];
	
	for i = 1:num_samples			
		tv = matshare.tests.common.vargen.variablegenerator(rns, maxDepth, maxElements, maxDims, maxChildren, true, typespec);
		
				
		switch(ceil(13*randdoubles1(i)))
			case 1
				matshare.share(tv);
			case 2
				data = matshare.share(tv);
			case 3
				[data, data1] = matshare.share(tv,tv);
			case 4
				matshare.share(tv,tv);
			case 5
				data = matshare.share(tv,tv);
			case 6
				matshare.share('-p',tv);
			case 7
				matshare.share('-p',tv,tv);
			case 8
				matshare.share('-n', 'n1', tv);
			case 9
				matshare.share('-n', 'n1', tv, 'n2', tv);
			case 10
				matshare.share('-n', 'n1', tv, 'n1', tv);
			case 11
				matshare.share('-p','-n','n1',tv);
			case 12
				matshare.share('-n','-p','n1',tv);
			case 13
				matshare.share('-p','-n','n1', tv,'n2',tv);
		end
		
		switch(ceil(10*randdoubles2(i)))
			case 1
				f = matshare.fetch;
			case 2
				f = matshare.fetch('-r');
			case 3
				f = matshare.fetch('-n');
			case 4
				f = matshare.fetch('-a');
			case 5
				f = matshare.fetch('-s');
			case 6
				[f1,f2] = matshare.fetch('-r','-n');
			case 7
				f = matshare.fetch('-w');
			case 8
				[f1,f2] = matshare.fetch('n1','n2');
			case 9
				f = matshare.fetch('-r','-n','-a');
			case 10
				f = matshare.fetch('-s','-n');
		end

		if(mod(i, mshclear_iter) == 0)
			if(randdoubles2(i) < 0.75 || numel(allvars) == 0)
				matshare.clear;
			else
				clridx = sort(randi(numel(allvars),[2,1]));
				matshare.clear(f.all(clridx(1):clridx(2)));
			end
			mshclear_iter = ceil(mshclear_bound*randdoubles3(i));
		end


		if(mod(i, mshlocalcopy_iter) == 0)
			if(~isempty(data))
				x = data.copy;
			end
		end

		if(mod(i, mshreset_iter) == 0)
			matshare.reset;
			mshreset_iter = ceil(mshreset_bound*randdoubles3(i));
		end

		if(mod(i, mshdetach_iter) == 0)
			matshare.detach;
			mshdetach_iter = ceil(mshdetach_bound*randdoubles3(i));
		end

		if(mod(i, clear_data_iter) == 0)
			f = [];
			clear_data_iter = ceil(clear_data_bound*randdoubles3(i));
		end

		if(mod(i, clear_new_iter) == 0)
			data = [];
			clear_new_iter = ceil(clear_new_bound*randdoubles3(i));
		end

		if(mod(i, chpar_gc_on_iter) == 0)
			matshare.config('GarbageCollection','on');
			chpar_gc_on_iter = ceil(chpar_gc_on_bound*randdoubles3(i));
		end

		if(mod(i, chpar_gc_off_iter) == 0)
			matshare.config('GarbageCollection','off');
			chpar_gc_off_iter = ceil(chpar_gc_off_bound*randdoubles3(i));
		end
		
	end
	
end