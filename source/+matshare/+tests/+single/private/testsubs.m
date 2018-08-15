function tv = testsubs(tv, f, subs, fcn)
	if(nargin < 4)
		tv(subs{:}) = rand(size(tv(subs{:})));
	else
		tv(subs{:}) = fcn(size(tv(subs{:})));
	end
	f.data(subs{:}) = tv(subs{:});

	if(~matshare.scripts.compstruct(tv, f.data))
		error('Subscripted assignment failed because results were not equal.');
	end
end

