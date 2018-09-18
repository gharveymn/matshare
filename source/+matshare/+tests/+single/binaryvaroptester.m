classdef binaryvaroptester
	properties
		mshf
		matf
	end
	
	methods
		function obj = binaryvaroptester(inmshf, inmatf)
			if(nargin > 0)
				obj.mshf = inmshf;
				obj.matf = inmatf;
			end
		end
		
		function test(obj,tv1,tv2)
			x = matshare.share(tv1);
			mshres = obj.mshf(x, tv2);
			matres = obj.matf(tv1, tv2);
			if(mshres ~= matres)
				error('Incorrect result');
			end
		end
	end
end

