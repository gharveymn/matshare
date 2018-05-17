function testrandfuncworker(maxDepth, maxElements, maxDims, maxChildren, typespec, num_samples, bounds, observerpid)
%	thispid = feature('getpid');
	allvars = {};
	rng('shuffle');
%	lents = 0
	
	for i = 1:num_samples
		try
			
			tv = randvargen(maxDepth, maxElements, maxDims, maxChildren, false, typespec);
			ri1 = ceil(4*rand);
			ri2 = ceil(4*rand);
			
			switch(ri1)
				case 1
					mshshare(tv);
				case 2
					data = mshshare(tv);
				case 3
					[data, newvars] = mshshare(tv);
				case 4
					[data, newvars, allvars] = mshshare(tv);
			end
			
			switch(ri2)
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