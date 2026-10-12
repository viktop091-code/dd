class AIConvoy_Car extends Offroad_02
{

    override bool CrewCanGetThrough(int posIdx)
    {
        return false;
    }
}

class AIConvoy_Truck extends Truck_01_Covered
{

    override bool CrewCanGetThrough(int posIdx)
    {
        return false;
    }
}
