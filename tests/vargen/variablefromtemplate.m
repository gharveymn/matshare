function out = variablefromtemplate(rns, in)
	out = in;
	if(isstruct(out))
		fn = fieldnames(out);
		for i = 1:numel(fn)
			for j = 1:numel(out)
				out(j).(fn{i}) = variablefromtemplate(rns, in(j).(fn{i}));
			end
		end
	elseif(iscell(in))
		for i = 1:numel(out)
			out{i} = variablefromtemplate(rns, in{i});
		end
	else
		out = rand(rns, size(in), 'like', in);
	end
end

