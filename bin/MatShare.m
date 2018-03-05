classdef MatShare < handle
    
	properties (Transient, Dependent, GetObservable, SetObservable)
		data
	end
	
	properties (Constant, Access=private)
		% These are just for documentation since it's faster to just use the numbers directly
		SHARE    = uint8(0);
        FETCH    = uint8(1);
		DETACH   = uint8(2);
        PARAM    = uint8(3);
		DEEPCOPY = uint8(4);
		DEBUG    = uint8(5);
	end
	
	methods
		function obj = MatShare
			% does nothing
		end
		
		function set.data(~,in)
			matshare_(MatShare.SHARE, in);
		end
		
		function out = get.data(~)
 			out = matshare_(MatShare.FETCH);
		end
		
		function ret = deepcopy(~)
			ret = matshare_(MatShare.DEEPCOPY);
		end
		
		function debug(~)
			matshare_(MatShare.DEBUG);
		end
		
		function delete(~)
			matshare_(MatShare.DETACH);
			clear matshare_
		end
		
	end
	
	methods (Static)
		
		function share(in)
			matshare_(MatShare.SHARE, in);
		end
		
		function out = fetch()
			% get callback
			out = matshare_(MatShare.FETCH);
        end
        
        function detach()
           matshare_(MatShare.DETACH); 
        end
		
	end
end

