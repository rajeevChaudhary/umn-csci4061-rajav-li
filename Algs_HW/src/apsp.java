package apsp;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;

public class apsp {

	public static final String ROOT_DIR = "./";
	public static final String OUTPUT_DIR = "output/";
	public static final String FINAL_APSP_FILE = "finalAPSP.txt";
	public static final String PATH_APSP_FILE = "pathAPSP.txt";
	
	public static void main(String[] args) {
		//System.out.println("Starting");
		
		if (args.length != 2) {
			System.err.println("Usage: apsp <Path to graph file> <Number of vertices in graph>");
			System.exit(1);
		}
		
		String graphFile = args[0];
		int numVertices = new Integer(args[1]);
		
		Graph G = new Graph();
		Vertex[] vertices = new Vertex[numVertices];
		
		for (int i = 0; i < numVertices; ++i) {
			Vertex v = new Vertex();
			v.index = i;
			vertices[i] = v;
			G.add(v);
		}
		
		try {
			BufferedReader buffer = new BufferedReader(new FileReader(graphFile));
			
			for (int i = 0; i < numVertices; ++i) {
				String currentLine;
				
				if ((currentLine = buffer.readLine()) == null) {
					System.err.println("Error in graph file format: Too few adjacency lists for specificed number of vertices");
					System.exit(1);
				}
				
				final String delimiter = "[,]";
				String[] adjList = currentLine.split(delimiter);
				
				if (adjList.length > 1 && ((adjList.length % 1) != 0)) {
					System.err.println("Error in graph file format: Adjacency list has vertex with no weight");
					System.exit(1);
				}
				
				for (int j = 1; j < adjList.length; j += 2) {
					Edge e = new Edge(vertices[i], vertices[new Integer(adjList[j-1]) - 1]);
					e.weight = new Integer(adjList[j]);
					G.add(e);
				}
			}
		} catch (FileNotFoundException e) {
			System.err.println("Graph file not found");
			System.exit(1);
		} catch (IOException e) {
			System.err.println("Could not read the graph file");
			System.exit(1);
		} catch (ArrayIndexOutOfBoundsException e) {
			System.err.println("Error in graph file: Invalid vertex " + e.getMessage());
			System.exit(1);
		}
		
		/*try {
			
			long[][] D = G.Johnson();
			for (int i = 0; i < numVertices; ++i) {
				for (int j = 0; j < numVertices; ++j) {
					if (D[i][j] == Long.MAX_VALUE)
						System.out.print("E");
					else
						System.out.print(D[i][j]);
					System.out.print(",");
				}
				System.out.println("");
			}
			
		} catch (Error e) {
			System.out.println("NC");
		}*/
		
		new File(ROOT_DIR + OUTPUT_DIR).mkdir();
		BufferedWriter finalFile = null;
		BufferedWriter pathFile = null;
		
		try {
			finalFile = new BufferedWriter(new FileWriter(ROOT_DIR + OUTPUT_DIR + FINAL_APSP_FILE));
			pathFile = new BufferedWriter(new FileWriter(ROOT_DIR + OUTPUT_DIR + PATH_APSP_FILE));
			
			long[][] D;
			
			try {
				D = G.Johnson();
				
				for (int i = 0; i < numVertices; ++i) {
					String row = "";
					
					for (int j = 0; j < numVertices; ++j) {
						if (D[i][j] == Long.MAX_VALUE)
							row += "E";
						else
							row += Long.toString(D[i][j]);
						
						if (j < numVertices - 1)
							row += ",";
					}
					
					finalFile.write(row);
					finalFile.newLine();
				}
				
				for (int i = 0; i < numVertices; ++ i) {
					String row = "";
					
					for (int j = 0; j < numVertices; ++j) {
						if (G.APSP[i][j] == null)
							row += "N";
						else
							row += Integer.toString(G.APSP[i][j].index + 1);
						
						if (j < numVertices - 1)
							row += ",";
					}
					
					pathFile.write(row);
					pathFile.newLine();
				}
				
			} catch (Graph.NegativeCycleException e) {
				finalFile.write("NC");
				finalFile.newLine();
				
				pathFile.write("NC");
				pathFile.newLine();
			} catch (Graph.InfiniteDistanceException e) {
				e.printStackTrace();
				System.exit(1);
			}
			
		} catch (IOException e) {
			System.err.println("Could not write to output files");
			System.exit(1);
		} finally {
            try {
                if (finalFile != null) {
                    finalFile.close();
                }
                if (pathFile != null) {
                	pathFile.close();
                }
            } catch (IOException e) {
                e.printStackTrace();
                System.exit(1);
            }
        }
		
		
	}

}
