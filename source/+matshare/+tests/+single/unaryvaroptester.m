classdef unaryvaroptester
	properties
		mshf
		matf
	end
	
	methods
		function obj = unaryvaroptester(inmshf, inmatf)
			if(nargin > 0)
				obj.mshf = inmshf;
				obj.matf = inmatf;
			end
		end
		
		function test(obj,tv)
			x = matshare.share(tv);
			mshres = obj.mshf(x);
			matres = obj.matf(tv);
			if(mshres ~= matres)
				error('Incorrect result');
			end
		end
	end
end