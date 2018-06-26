function shared_variable = mshpersistshare(in)
	if(nargout == 0)
		matshare_(14, in);
	else
		shared_variable = matshare_(14, in);
		matshare_(12);
	end
end

