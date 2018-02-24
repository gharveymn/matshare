classdef mshdata < handle
	
	properties (AbortSet, Access=private, Transient)
		deepdata
	end
	
	properties (Dependent, GetObservable, SetObservable)
		data
	end
	
	properties (Constant, Access=private, Hidden)
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
		function obj = mshdata
			obj.deepdata = matshare_(obj.INIT);
			addlistener(obj, 'data', 'PreSet', @(src,evnt)mshdata.preSetCallback(obj,src,evnt));	
			addlistener(obj, 'data', 'PreGet', @(src,evnt)mshdata.preGetCallback(obj,src,evnt));	
		end
		
		function set.data(obj,in)
			obj.deepdata = matshare_(obj.CLONE,in);
			matshare_(obj.ATTACH);
		end
		
		function ret = get.data(obj)
 			ret = matshare_(obj.COPY);
% 			ret = obj.deepdata;
		end
		
		function ret = deepcopy(obj)
			ret = matshare_(obj.DEEPCOPY);
		end
		
		function debug(obj)
			matshare_(obj.DEBUG);
		end
		
		function delete(obj)
			obj.deepdata = matshare_(obj.DETACH);
			clear matshare_ obj.deepdata;
		end
		
	end
	
	methods (Static)
		function preSetCallback(obj,~,~)
			obj.deepdata = matshare_(obj.DETACH);
		end
		
		function preGetCallback(obj,~,~)
			if(~matshare_(obj.COMPARE, obj.deepdata))
				obj.deepdata = matshare_(obj.DETACH);
				obj.deepdata = matshare_(obj.FETCH);
				matshare_(obj.ATTACH);
			end
		end
	end
end

