classdef MatShare < handle
	
	properties (AbortSet, Access=private, Transient)
		deepdata
	end
	
	properties (Transient, Dependent, GetObservable, SetObservable)
		data
	end
	
	properties (Constant)
		% These are just for documentation since it's faster to just use the numbers directly
		INIT     = uint8(0);
		CLONE    = uint8(1);
		ATTACH   = uint8(2);
		DETACH   = uint8(3);
		FREE     = uint8(4);
		FETCH    = uint8(5);
		COMPARE  = uint8(6);
		COPY     = uint8(7);
		DEEPCOPY = uint8(8);
		DEBUG    = uint8(9);
	end
	
	methods
		function obj = MatShare(varargin)
			if(nargin > 0 && strcmp('unsafe',varargin{1}))
				% initialize as unsafe
				matshare_(uint8(0), 1);
			else
				matshare_(uint8(0));
			end
% 			addlistener(obj, 'data', 'PreSet', @(src,evnt)MatShare.preSetCallback(obj,src,evnt));	
% 			addlistener(obj, 'data', 'PreGet', @(src,evnt)MatShare.preGetCallback(obj,src,evnt));	
		end
		
		function set.data(~,in)
			matshare_(uint8(1), in);
		end
		
		function out = get.data(~)
			% make sure this isn't deep-copied
 			out = matshare_(uint8(5));
		end
		
		function ret = deepcopy(~)
			ret = matshare_(uint8(8));
		end
		
		function debug(~)
			matshare_(uint8(9));
		end
		
		function safefree(~)
			matshare_(uint8(4));
		end
		
		function delete(~)
			matshare_(uint8(4));
			clear matshare_ obj.deepdata;
		end
		
	end
	
	methods (Static)
% 		function preSetCallback(obj,~,~)
% 			obj.deepdata = matshare_(uint8(3));
% 		end
% 		
% 		function preGetCallback(obj,~,~)
% 			[obj.deepdata,ischanged] = matshare_(uint8(5));
% 			if(ischanged)
% 				matshare_(uint8(2));
% 			end
% 		end
		
		function init(varargin)
			if(nargin > 0 && strcmp('unsafe',varargin{1}))
				% initialize as unsafe
				matshare_(uint8(0), 1);
			else
				matshare_(uint8(0));
			end
		end
		
		function share(in)
			matshare_(uint8(1), in);
		end
		
		function out = fetch()
			% get callback
			out = matshare_(uint8(5));
		end
		
		function free()
			matshare_(uint8(4));
			clear matshare_
		end
		
	end
end

