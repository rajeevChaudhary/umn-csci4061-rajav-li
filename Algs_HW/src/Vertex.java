package apsp;

public class Vertex implements Comparable<Vertex> {

	public int index;
	
	public long distance; // Sentinel value Long.MAX_VALUE (2^63-1) signifies infinity (this vertex is unreachable from the current source)
	public Vertex predecessor;
	
	// Method required to implement Comparable interface, used for PriorityQueue
	public int compareTo(Vertex v) {
		if (distance < v.distance)
			return -1;
		else if (distance == v.distance)
			return 0;
		else
			return 1;
	}
	
}
