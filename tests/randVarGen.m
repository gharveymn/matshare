function [ret] = randVarGen(maxDepth, maxElements, ignoreUnusables)
	[ret] = randVarGen_(maxDepth, 1, maxElements, ignoreUnusables);
end


function [ret] = randVarGen_(maxDepth, currDepth, maxElements, ignoreUnusables)

	%  Variable Type Key
	% 	1	mxLOGICAL_CLASS,
	% 	2	mxCHAR_CLASS,
	% 	3	mxVOID_CLASS,
	% 	4	mxDOUBLE_CLASS,
	% 	5	mxSINGLE_CLASS,
	% 	6	mxINT8_CLASS,
	% 	7	mxUINT8_CLASS,
	% 	8	mxINT16_CLASS,
	% 	9	mxUINT16_CLASS,
	% 	10	mxINT32_CLASS,
	% 	11	mxUINT32_CLASS,
	% 	12	mxINT64_CLASS,
	% 	13	mxUINT64_CLASS,
	% 	14	mxFUNCTION_CLASS,
	% 	15	mxOPAQUE_CLASS/SPARSES,
	% 	16	mxOBJECT_CLASS,
	% 	17	mxCELL_CLASS,
	% 	18	mxSTRUCT_CLASS

	if(maxDepth <= currDepth)
		%dont make another layer
		vartypegen = randi(16) + 2;
		%vartypegen = 17;
	else
		vartypegen = randi(2);
		%vartypegen = 2;
	end

	if(vartypegen > 2)
		thisMaxElements = maxElements;
	else
		%max elems for structs
		thisMaxElements = 16;
	end

	numvarsz = randi(thisMaxElements + 1) - 1;
	nums = 1:numvarsz;
	divs = nums(mod(numvarsz, nums) == 0);
	temp = numvarsz;
	dims = {};
	if(numvarsz ~= 0)
		while(true)
			randdiv = divs(randi(numel(divs)));
			dims = [dims {randdiv}];
			temp = temp / randdiv;
			divs = divs(mod(temp,divs) == 0);
			if(temp == 1)
				break;
			end
		end
	else
		ndims = randi(32);
		adims = zeros(1,ndims);
		dims = num2cell(adims);
	end

	if(numel(dims) == 1)
		dims = [dims {1}];
	end

	switch(vartypegen)
		case(1)
			% 	1	mxCELL_CLASS,
			ret = cell(dims{:});
			for k = 1:numel(ret)

				ret{k} = randVarGen_(maxDepth, ...
					currDepth + 1, ...
					maxElements, ...
					ignoreUnusables);
				
			end
		case(2)
			% 	2	mxSTRUCT_CLASS
			numPossibleFields = 5;
			if(numvarsz ~= 0)
				possibleFields = {'cat',[],'dog',[],'fish',[],'cow',[],'twentyonesavage',[]};
				ret(dims{:}) = struct(possibleFields{1:2*randi(numPossibleFields)});
				retFields = fieldnames(ret);
				for k = 1:numel(ret)

					for j = 1:numel(retFields)
						ret(k).(retFields{j}) = randVarGen_(maxDepth, currDepth + 1, ...
							maxElements, ignoreUnusables);

					end

				end
			else
				possibleFields = {'cat',dims,'dog',dims,'fish',dims,'cow',dims,'twentyonesavage',dims};
				ret = struct(possibleFields{1:2*randi(numPossibleFields)});
			end

		case(3)
			% 	3	mxLOGICAL_CLASS,
			ret = logical(rand(dims{:}) > 0.5);
		case(4)
			% 	4	mxCHAR_CLASS,
			ret = char(randi(65536,dims{:})-1);
		case(5)
			% 	5	mxVOID_CLASS/SPARSE,
			%reserved, make a double instead or sparse
			if(numel(dims) == 2)
				ret = sparse(logical(rand(dims{:}) > 0.5));
			else
				ret = rand(dims{:},'double');
			end
		case(6)
			% 	6	mxDOUBLE_CLASS,
			ret = rand(dims{:},'double');
		case(7)
			% 	7	mxSINGLE_CLASS,
			ret = rand(dims{:},'single');
		case(8)
			% 	8	mxINT8_CLASS,
			ret = randi(intmax('int8'),dims{:},'int8');
		case(9)
			% 	9	mxUINT8_CLASS,
			ret = randi(intmax('uint8'),dims{:},'uint8');
		case(10)
			% 	10	mxINT16_CLASS,
			ret = randi(intmax('int16'),dims{:},'int16');
		case(11)
			% 	11	mxUINT16_CLASS,
			ret = randi(intmax('uint16'),dims{:},'uint16');
		case(12)
			% 	12	mxINT32_CLASS,
			ret = randi(intmax('int32'),dims{:},'int32');
		case(13)
			% 	13	mxUINT32_CLASS,
			ret = randi(intmax('uint32'),dims{:},'uint32');
		case(14)
			% 	14	mxINT64_CLASS,
			f = @() uint64( randi(intmax('uint32'), dims{:}, 'uint32') );
			% bitshift and bitor to convert into a proper uint64        
			ret = int64(bitor( bitshift(f(),32), f() ));
		case(15)
			% 	15	mxUINT64_CLASS,
			f = @() uint64( randi(intmax('uint32'), dims{:}, 'uint32') );
			% bitshift and bitor to convert into a proper uint64        
			ret = bitor( bitshift(f(),32), f() );
		case(16)
			% 	16	mxFUNCTION_CLASS,
			if(ignoreUnusables)
				ret = rand(dims{:},'double');
			else
				af1 = @(x) x + 17;
				af2 = @(x,y,z) x*y*z;
				sf1 = @simplefunction1;
				sf2 = @simplefunction2;

				funcs = {af1,af2,sf1,sf2};
				ret = funcs{randi(numel(funcs))};
			end
		case(17)
			% 	17	mxOPAQUE_CLASS,
			% not sure how to generate, generate a sparse array instead
			if(numel(dims) == 2)
				if(rand > 0.5)
					ret = sparse(logical(rand(dims{:}) > 0.5).*rand(dims{:},'double'));
					if(rand > 0.5)
						ret = ret + 1i*sparse(logical(rand(dims{:}) > 0.5).*rand(dims{:},'double'));
					end
				else
					ret = sparse(logical(rand(dims{:}) > 0.5));
				end
			else
				ret = rand(dims{:},'double') +  1i*rand(dims{:},'double');
			end
		case(18)
			% 	18	mxOBJECT_CLASS,
			if(ignoreUnusables)
				ret = rand(dims{:},'double') + 1i*rand(dims{:},'double');
			else
				ret = BasicClass(randi(intmax),dims);
			end

	end

end

