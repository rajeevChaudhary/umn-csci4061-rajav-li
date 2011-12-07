package apsp;

public class Edge {

	public Vertex source; // The 'u' in (u, v)
	public Vertex destination; // The 'v' in (u, v)
	
	public long weight;
	
	public Edge(Edge e) {
		source = e.source;
		destination = e.destination;
		weight = e.weight;
	}
	
	public Edge(Vertex u, Vertex v) {
		source = u;
		destination = v;
	}
	
	public Edge(Vertex u, Vertex v, int w) {
		this(u, v);
		weight = w;
	}
	
	public boolean relax() {
		// If u.d = infinity, the comparison v.d > u.d + (u, v).w will never be true
		// We need to check for that first because Long.MAX_VALUE + any positive integer will overflow
		if (source.distance != Long.MAX_VALUE && destination.distance > source.distance + weight) {
			destination.distance = source.distance + weight;
			destination.predecessor = source;
			return true;
		}
		return false;
	}
	
}
