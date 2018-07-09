function out = variablefromtemplate(rns, in)
	out = in;
	if(isstruct(out))
		fn = fieldnames(out);
		for i = 1:numel(fn)
			for j = 1:numel(out)
				out(j).(fn{i}) = matshare.tests.common.vargen.variablefromtemplate(rns, in(j).(fn{i}));
			end
		end
	elseif(iscell(out))
		for i = 1:numel(out)
			out{i} = matshare.tests.common.vargen.variablefromtemplate(rns, in{i});
		end
	elseif(isnumeric(out))
		
		if(issparse(in))
			out = sprand(in);
		else
			if(numel(in) > 0)
				if(isa(in, 'double') || isa(in, 'single'))
					out = rand(rns, size(in), class(in));
				elseif(isa(in, 'int64'))
					out = int64(bitor(bitshift(randuint64shifted(rns, size(in)),32), randuint64shifted(rns, size(in))));
				elseif(isa(in, 'uint64'))
					out = bitor(bitshift(randuint64shifted(rns, size(in)),32), randuint64shifted(rns, size(in)));
				else
					out = randi(rns, max(in(:)), size(in), class(in));
				end
			end
		end
	end
end

