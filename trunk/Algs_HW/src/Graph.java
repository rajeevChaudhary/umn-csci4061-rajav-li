package apsp;

import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.PriorityQueue;
import java.util.Set;

public class Graph {

	public Set<Vertex> vertices;
	public Set<Edge> edges;
	
	public Map<Vertex, Set<Edge>> adj; // Adjacency lists, updated automatically in add() methods (map keyed on edge source vertices)
	
	public Vertex[][] APSP; // APSP predecessor matrix, run Johnson() to generate
	
	@SuppressWarnings("serial") public class NegativeCycleException extends Exception {}
	@SuppressWarnings("serial") public class InfiniteDistanceException extends Exception {}
	
	public Graph() {
		vertices = new LinkedHashSet<Vertex>();
		edges = new LinkedHashSet<Edge>();
		adj = new HashMap<Vertex, Set<Edge>>();
	}
	
	// Copy over all vertex, edge, and adj. list references from existing graph
	// New containing sets and maps are created, but all vertices, edges, and adj. lists are copied by REFERENCE, not VALUE
	public Graph(Graph G) {
		vertices = new LinkedHashSet<Vertex>(G.vertices);
		edges = new LinkedHashSet<Edge>(G.edges);
		adj = new HashMap<Vertex, Set<Edge>>(G.adj);
	}
	
	// Since vertices and edges are of type Set, the two add methods
	
	public void add(Vertex v) {
		if (vertices.add(v)) { // True if v was actually added (was not already in the Set)
			adj.put(v, new LinkedHashSet<Edge>()); // Create new adjacency list for vertex v
		}
	}
	
	public void add(Edge e) throws IllegalArgumentException {
		if (!vertices.contains(e.source)) {
			throw new IllegalArgumentException("Could not add edge: Source vertex not in graph");
		} else if (!vertices.contains(e.destination)) {
			throw new IllegalArgumentException("Could not add edge: Destination vertex not in graph");
		} else {
			if (edges.add(e)) { // True if e was actually added (was not already in the Set)
				adj.get(e.source).add(e); // Add edge (u, v) to vertex u's adjacency list
			}
		}
	}
	
	
	public void initializeSingleSource(Vertex source) {
		// for each Vertex v in vertices
		for (Iterator<Vertex> iter = vertices.iterator(); iter.hasNext();) {
			Vertex v = iter.next();
			
			v.distance = Long.MAX_VALUE; // Sentinel value to signify infinity
			v.predecessor = null;
		}
		
		source.distance = 0;
	}
	
	public void BellmanFord(Vertex source) throws NegativeCycleException {
		initializeSingleSource(source);
		
		// for i = 1 to |V|-1
		for (int i = 1; i < vertices.size(); ++i) {
			// for each Edge e in edges
			for (Iterator<Edge> iter = edges.iterator(); iter.hasNext();) {
				Edge e = iter.next();
				
				e.relax();
			}
		}
		
		// for each Edge e in edges
		for (Iterator<Edge> iter = edges.iterator(); iter.hasNext();) {
			Edge e = iter.next();
			
			// If u.d = infinity, the comparison v.d > u.d + (u, v).w will never be true
			// We need to check for that first because Long.MAX_VALUE + any positive integer will overflow
			if (e.source.distance != Long.MAX_VALUE && e.destination.distance > e.source.distance + e.weight) {
				throw new NegativeCycleException();
			}
		}
	}
	
	public void Dijkstra(Vertex source) {
		initializeSingleSource(source);
		
		//Set<Vertex> done = new LinkedHashSet<Vertex>(); <-- Not needed?
		
		// Initializes min-priority queue keyed on 'distance' field of Vertex
		// PriorityQueue utilizes natural ordering through Comparable interface 
		PriorityQueue<Vertex> Q = new PriorityQueue<Vertex>(vertices);
		
		while (!Q.isEmpty()) {
			Vertex v = Q.poll(); // EXTRACT-MIN
			
			//done.add(u);
			
			// for each Edge e in the adjacency list of Vertex v
			for (Iterator<Edge> iter = adj.get(v).iterator(); iter.hasNext();) {
				Edge e = iter.next();
				
				if (e.relax()) { // True if relaxation actually happened
					// We DECREASE-KEY on Q by deleting and re-inserting the element
					if (Q.contains(e.destination)) {
						Q.remove(e.destination);
						Q.add(e.destination);
					}
				}
			}
		}
	}
	
	public long[][] Johnson() throws NegativeCycleException, InfiniteDistanceException {
		long[][] D = new long[vertices.size()][vertices.size()];
		APSP = new Vertex[vertices.size()][vertices.size()];
		
		Vertex s = new Vertex();
		s.index = vertices.size();
		
		Graph Gprime = new Graph(this); // Copy this graph
		Gprime.add(s);
		// Make 0-weight edges from s to all vertices
		for (Vertex v : vertices) {
			Edge e = new Edge(s, v);
			e.weight = 0;
			Gprime.add(e);
		}
		
		Gprime.BellmanFord(s); // Throws NegativeCycleException
		
		long[] h = new long[Gprime.vertices.size()];
		
		for (Vertex v : vertices) {
			h[v.index] = v.distance; // distance computed in Bellman-Ford
		}
		
		// for each Edge e in Gprime.edges
		for (Iterator<Edge> iter = Gprime.edges.iterator(); iter.hasNext();) {
			Edge e = iter.next();
			
			// This will only happen if there is something horribly wrong with our Bellman-Ford implementation,
			// since the algorithm will never leave infinite distances from our source vertex s which is
			// strongly connected to every other vertex in the graph
			if (e.source.distance == Long.MAX_VALUE || e.destination.distance == Long.MAX_VALUE)
				throw new InfiniteDistanceException();
			
			e.weight = e.weight + e.source.distance - e.destination.distance;
		}
		
		// for each Vertex u in vertices
		for (Iterator<Vertex> iter = vertices.iterator(); iter.hasNext();) {
			Vertex u = iter.next();
			
			Dijkstra(u);
			
			// for each Vertex v in vertices
			for (Iterator<Vertex> inner_iter = vertices.iterator(); inner_iter.hasNext();) {
				Vertex v = inner_iter.next();
				
				D[u.index][v.index] = v.distance == Long.MAX_VALUE ? Long.MAX_VALUE : v.distance + h[v.index] - h[u.index];
				APSP[u.index][v.index] = v.predecessor;
			}
		}
		
		return D;
	}
	
}
