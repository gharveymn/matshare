function ret = testrsa(x1, x2)
    if(isfloat(x1))
        ret = x1./2.^x2;
    else
       ret = cast(floor(double(x1)./2.^double(x2)), class(x1)); 
    end
end

