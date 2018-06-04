function sample_num = testprecompwkr(rns, maxDepth, maxElements, maxDims, maxChildren, typespec, num_samples, bounds, tv, sample_num)

	allvars = {};

	% bound of clearing of data
	bounds.clear_data.iter = randi(rns, bounds.clear_data.bound);

	bounds.clear_new.iter = randi(rns, bounds.clear_new.bound);

	bounds.clear_all.iter = randi(rns, bounds.clear_all.bound);

	% bound of random call to mshclear
	bounds.mshclear.iter = randi(rns, bounds.mshclear.bound);

	bounds.mshdetach.iter = randi(rns, bounds.mshdetach.bound);

	% bound of random call to mshconfig to set to copy-on-write
	bounds.chpar_copy.iter = randi(rns, bounds.chpar_copy.bound);

	% bound of random call to mshconfig to set to overwrite
	bounds.chpar_over.iter = randi(rns, bounds.chpar_over.bound);

	% bound of random call to mshconfig to set gc on
	bounds.chpar_gc_on.iter = randi(rns, bounds.chpar_gc_on.bound);

	% bound of random call of mshconfig to set gc off
	bounds.chpar_gc_off.iter = randi(rns, bounds.chpar_gc_off.bound);
	
	ri1 = ceil(4*rand(rns,[num_samples,1]));
	ri2 = ceil(4*rand(rns,[num_samples,1]));
	
	for i = 1:num_samples
		try
			switch(ri1(i))
				case 1
					mshshare(tv{sample_num});
				case 2
					data = mshshare(tv{sample_num});
				case 3
					[data, newvars] = mshshare(tv{sample_num});
				case 4
					[data, newvars, allvars] = mshshare(tv{sample_num});
			end
			
			switch(ri2(i))
				case 1
					mshfetch;
				case 2
					data = mshfetch;
				case 3
					[data, newvars] = mshfetch;
				case 4
					[data, newvars, allvars] = mshfetch;
			end
			
			if(mod(i, bounds.mshclear.iter) == 0)
				if(rand(rns) < 0.75 || numel(allvars) == 0)
					mshclear;
				else
					clridx = sort(randi(numel(allvars),[2,1]));
					mshclear(allvars{clridx(1):clridx(2)});
				end
				bounds.mshclear.iter = randi(rns, bounds.mshclear.bound);
			end
			
			
			if(mod(i, bounds.mshdetach.iter) == 0)
				mshdetach;
				bounds.mshdetach.iter = randi(rns, bounds.mshdetach.bound);
			end
			
			if(mod(i, bounds.clear_data.iter) == 0)
				data = [];
				bounds.clear_data.iter = randi(rns, bounds.clear_data.bound);
			end
			
			if(mod(i, bounds.clear_new.iter) == 0)
				newvars = [];
				bounds.clear_new.iter = randi(rns, bounds.clear_new.bound);
			end
			
			if(mod(i, bounds.clear_all.iter) == 0)
				allvars = [];
				bounds.clear_all.iter = randi(rns, bounds.clear_all.bound);
			end
			
			if(mod(i, bounds.chpar_copy.iter) == 0)
				mshconfig('sharetype', 'copy');
				bounds.chpar_copy.iter = randi(rns, bounds.chpar_copy.bound);
			end
			
			if(mod(i, bounds.chpar_over.iter) == 0)
				mshconfig('sharetype','overwrite');
				bounds.chpar_over.iter = randi(rns, bounds.chpar_over.bound);
			end
			
			if(mod(i, bounds.chpar_gc_on.iter) == 0)
				mshconfig('GC','on');
				bounds.chpar_gc_on.iter = randi(rns, bounds.chpar_gc_on.bound);
			end
			
			if(mod(i, bounds.chpar_gc_off.iter) == 0)
				mshconfig('GC','off');
				bounds.chpar_gc_off.iter = randi(rns, bounds.chpar_gc_off.bound);
			end
			
		catch mexcept
			if(~strcmp(mexcept.identifier,'matshare:InvalidTypeError'))
				rethrow(mexcept);
			end
		end
		sample_num = sample_num + 1;
	end
	
end