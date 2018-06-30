function rns = testrandfuncworker(maxDepth, maxElements, maxDims, maxChildren, typespec, num_samples, bounds, isparallel)
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
	
	for i = 1:num_samples
		try
			
			tv = variablegenerator(rns, maxDepth, maxElements, maxDims, maxChildren, false, typespec);
			
			switch(ceil(2*randdoubles1(i)))
				case 1
					mshshare(tv);
				case 2
					data = mshshare(tv);
			end
			
			switch(ceil(4*randdoubles2(i)))
				case 1
					mshfetch;
				case 2
					data = mshfetch;
				case 3
					[data, newvars] = mshfetch;
				case 4
					[data, newvars, allvars] = mshfetch;
			end
			
			if(mod(i, mshclear_iter) == 0)
				if(randdoubles2(i) < 0.75 || numel(allvars) == 0)
					mshclear;
				else
					clridx = sort(randi(numel(allvars),[2,1]));
					mshclear(allvars{clridx(1):clridx(2)});
				end
				mshclear_iter = ceil(mshclear_bound*randdoubles3(i));
			end
			
			
			if(mod(i, mshlocalcopy_iter) == 0)
				switch(ceil(9*randdoubles2(i)))
					case 1
						mshlocalcopy;
					case 2
						data = mshlocalcopy;
					case 3
						[data, newvars] = mshlocalcopy;
					case 4
						[data, newvars, allvars] = mshlocalcopy;
					case 5
						data = mshlocalcopy(data);
					case 6
						newvars = mshlocalcopy(newvars);
					case 7
						allvars = mshlocalcopy(allvars);
					case 8
						mshlocalcopy(data);
				end
			end
			
			if(mod(i, mshreset_iter) == 0)
				mshreset;
				mshreset_iter = ceil(mshreset_bound*randdoubles3(i));
			end
			
			if(mod(i, mshdetach_iter) == 0)
				mshdetach;
				mshdetach_iter = ceil(mshdetach_bound*randdoubles3(i));
			end
			
			if(mod(i, clear_data_iter) == 0)
				data = [];
				clear_data_iter = ceil(clear_data_bound*randdoubles3(i));
			end
			
			if(mod(i, clear_new_iter) == 0)
				newvars = [];
				clear_new_iter = ceil(clear_new_bound*randdoubles3(i));
			end
			
			if(mod(i, clear_all_iter) == 0)
				allvars = [];
				clear_all_iter = ceil(clear_all_bound*randdoubles3(i));
			end
			
			if(mod(i, chpar_gc_on_iter) == 0)
				mshconfig('GarbageCollection','on');
				chpar_gc_on_iter = ceil(chpar_gc_on_bound*randdoubles3(i));
			end
			
			if(mod(i, chpar_gc_off_iter) == 0)
				mshconfig('GarbageCollection','off');
				chpar_gc_off_iter = ceil(chpar_gc_off_bound*randdoubles3(i));
			end
			
		catch mexcept
			if(~strcmp(mexcept.identifier,'matshare:InvalidTypeError'))
				rethrow(mexcept);
			end
		end
		
	end
	
end